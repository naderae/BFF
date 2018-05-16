from django.shortcuts import render, get_object_or_404, redirect
from django.urls import reverse
from django.http import HttpResponse
from groups.models import Group
from .models import Post
from .forms import PostForm
from django.utils import timezone


# Create your views here.
def create(request, group_id):
    group = get_object_or_404(Group, pk= group_id)
    if request.method == 'POST':
        if request.POST['body'] and request.POST['title']:
            post = Post()
            post.title = request.POST['title']
            post.body = request.POST['body']
            post.pub_date = timezone.datetime.now()
            post.author = request.user
            post.group = group
            post.save()
            return redirect('/groups/' + str(group_id))

        else:
            return render(request, 'groups/detail.html', {'group':group})

    else:
        return render(request, 'groups/detail.html', {'group':group})

def edit(request, group_id, post_id):

    group = get_object_or_404(Group, pk= group_id)
    post =  get_object_or_404(Post, pk= post_id)
    if request.method == "POST":
        form = PostForm(request.POST, instance=post)
        if form.is_valid():
            post = form.save(commit=False)
            post.author = request.user
            post.group = group
            post.pub_date = timezone.now()
            post.save()
            return redirect('/groups/' + str(group_id))
    else:
        form = PostForm(instance=post)
    return render(request, 'posts/post_form.html', {'form':form})

def delete(request, group_id, post_id):
    group = get_object_or_404(Group, pk= group_id)
    post =  get_object_or_404(Post, pk= post_id)

    post.delete()
    return redirect('/groups/' + str(group_id))

def like(request, post_id, group_id):
    group = get_object_or_404(Group, pk= group_id)
    post =  get_object_or_404(Post, pk= post_id)
    posts = Post.objects.filter(group__id = group_id)

    if request.method == 'POST':
        if request.user in post.likers.all():
            # return reverse('groups:detail', kwargs={'group_id': group_id})
            # return redirect('groups:detail', group_id=group_id )
            return render(request, 'groups/detail.html', {'group':group, 'posts':posts} )
        else:
            post.likes_total += 1
            # with add, we are adding a liker to the likers relation, between a user and a post
            post.likers.add(request.user)
            post.save()
            return redirect('groups:detail', group_id=group_id )
    else:
        return render(request, 'groups/detail.html', {'group':group, 'posts':posts})
