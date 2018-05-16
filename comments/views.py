
from django.shortcuts import render, redirect, get_object_or_404
from django.contrib.auth.decorators import login_required
from .models import Comment
from posts.models import Post
from groups.models import Group
from django.contrib.auth.models import User
from .forms import CommentForm
from django.utils import timezone
# Create your views here.


def create(request,group_id,post_id):
    posts = Post.objects.filter(group__id = group_id)
    post =  get_object_or_404(Post, pk= post_id)
    group = post.group

    if request.method == "POST":
        if request.POST['body']:
            comment = Comment()
            comment.body = request.POST['body']
            comment.author = request.user
            comment.post = post
            comment.group = group
            comment.pub_date = timezone.now()
            comment.save()
            return redirect('/groups/' + str(group.id))
        else:
            return render(request,'groups/detail.html', {'group':group, 'posts':posts, 'error':'you must write something in the comment box'} )
    else:
        return render(request, 'groups/detail.html', {'group':group, 'posts':posts} )
    return render(request, 'groups/detail.html', {'group':group, 'posts':posts} )

def delete(request, group_id, post_id, comment_id):
    group = get_object_or_404(Group, pk= group_id)
    post =  get_object_or_404(Post, pk= post_id)
    comment = get_object_or_404(Comment, pk= comment_id)

    comment.delete()
    return redirect('/groups/' + str(group_id))
