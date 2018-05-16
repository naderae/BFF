from django.db import models
from posts.models import Post
from groups.models import Group
from django.contrib.auth.models import User
# Create your models here.

class Comment(models.Model):
    body = models.TextField(blank=True, default='')
    likes_total = models.IntegerField(default=0)
    pub_date = models.DateTimeField()
    author = models.ForeignKey(User, on_delete=models.CASCADE)
    post = models.ForeignKey(Post, on_delete=models.CASCADE)
    group = models.ForeignKey(Group, on_delete=models.CASCADE)

    def __str__(self):
        return self.body


    def pub_date_pretty(self):
        # strftime is how to break down time
        return self.pub_date.strftime('%b %e %Y')
